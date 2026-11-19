# Vrutti IDE — Implementation Task Roadmap

**Scope:** `vrutti_ide/` only. Parent `vrutti/` (upstream VS Code source) is never modified.

**Method:** Quality audit performed on every `.cpp`, `.h`, `.js`, and `.ts` file in `vrutti_ide/src/`. Findings:
- **Real implementation:** `main.cpp`, `IPCClient`, `VSCodeProtocol` (partial), `DocumentSyncService` (Piece Table + 50ms debounce), `ExtensionScanner` (reads package.json + base64 icons), `VSCodeExtHostService` (builds `IExtensionHostInitData`), `VSXRegistryService` (Open VSX fetch + LRU cache), `FileSystemService` (stat/read/write/list, but content returns `"..."`), `PieceTable`, `ArenaAllocator`, `ThreadPool`, `Json`, `Base64`, `Window.cpp` (WebView2 shell, but insecure).
- **Stub/dummy:** `TerminalService` (mock reader thread sleeps 500ms then exits — no real PTY), `DebugAdapterService` (mock DAP, no real adapter), `LanguageFeatureService` (empty completions/hover), `CommandHandlerService` (registry only), `ExtensionLifecycleManager` (activation stub), `DiagnosticService` (simplified parse), `WorkspaceService` (update does nothing), `MediaController` (no-op), `ExtensionManager` (list only), `SettingsManager` (blocking `saveAsync().wait()` while holding mutex — deadlock), `VSCodeProtocol` ("...we will expand this with a full router map"), `IPCClient` (string-concatenated JSON, string-match method routing, no promise resolver).
- **Missing entirely:** extension install/uninstall (security hole via `_popen`), save-as dialog, split editor groups, minimap, breadcrumbs, diff editor wiring, notebook editor, welcome page, auto-update, telemetry, crash reporter, secret storage, remote development, notifications, progress, quick input, per-instance pipe name, sandboxed extension iframes, DI container.

**Execution order:** Backend → Frontend → Polish → Release. One release ≈ one phase. Target: 10–20 releases to a compilable, distributable dev-stage app.

---

## Phase 1 — Foundation & Build Hygiene (Release 1)

**Goal:** Make the C++ core and frontend reliably compile and link together; establish a single build entry.

| # | Task | Area | Priority | Done? |
|---|------|------|----------|-------|
| 1.1 | Audit `CMakeLists.txt` and `Makefile`; ensure all 16 plugin DLLs + core libs + `vrutti_app.exe` are build targets with consistent output dir (`build/`) | Build | Critical | ⬜ |
| 1.2 | Add `nlohmann/json` (or equivalent) dependency to CMake; replace ad-hoc `Json.h`/`Json.cpp` string concatenation with a maintained JSON library | Core Utils | Critical | ⬜ |
| 1.3 | Fix `SettingsManager::saveAsync()` — remove `m_saveFuture.wait()` while holding `mutex_`; push writes to a dedicated background worker thread queue | Core Config | Critical | ⬜ |
| 1.4 | Fix `IPCClient` — replace string-concatenated JSON with proper serialization; replace naive `jsonMessage.find("\"method\":\"insert\"")` with real JSON deserialization | Core IPC | Critical | ⬜ |
| 1.5 | Fix `PluginLoader` — add `std::mutex` + `lock_guard` around all `m_loadedPlugins` map accesses (race condition) | Core Plugins | Critical | ⬜ |
| 1.6 | Verify `PieceTable.cpp` is complete and tested; add unit tests for insert/remove/undo | Core Editor | High | ⬜ |
| 1.7 | Verify `ArenaAllocator`, `ThreadPool`, `Async`, `CancellationToken` compile and link | Core Utils | High | ⬜ |
| 1.8 | Frontend: verify `vite.config.ts`, `tsconfig.json` build cleanly; run `npm install` + `npm run build` once | UI Frontend | High | ⬜ |

**Release 1 exit criteria:** `cmake --build build` produces `vrutti_app.exe` + all plugin DLLs without errors; `npm run build` in `src/ui/frontend` produces `dist/` without errors.

---

## Phase 2 — Security Hardening (Release 2)

**Goal:** Eliminate all critical RCE/XSS vulnerabilities catalogued in `problem.md`.

| # | Task | Area | Priority | Done? |
|---|------|------|----------|-------|
| 2.1 | Remove `_popen` shell execution in `Window.cpp` extension installer; replace with `libcurl` (download) + `libzip` (unpack) native C++ | UI Compositor | Critical | ⬜ |
| 2.2 | Fix `vruttiGitCommand` / `vruttiRunFile` — replace shell string concat (`cd /d "..." && cmd`) with `CreateProcess`/`execvp` argument arrays | UI Compositor | Critical | ⬜ |
| 2.3 | Remove `eval(...)` in IPC handlers; escape all Webview parameters via `JsonSerializer::escapeString` before embedding in JS string literals | UI Compositor | Critical | ⬜ |
| 2.4 | Sandbox extension CSS/JS injections (`extensions/injections`) — move to iframe + `postMessage`; remove direct `style.innerHTML` / `script.innerHTML` into main DOM | UI Frontend | Critical | ⬜ |
| 2.5 | Sanitize all Lit `unsafeHTML(item.iconContent)` with `DOMPurify` before rendering | UI Frontend | Critical | ⬜ |
| 2.6 | Replace `decodeURIComponent(escape(atob(b64)))` base64 UTF-8 polyfill with `new TextDecoder().decode()` | UI Frontend | High | ⬜ |
| 2.7 | Fix hardcoded IPC pipe `vrutti_pipe` → per-instance `vrutti_pipe_<PID>` in `main.cpp` and `bootstrapper.js` | App Entry | High | ⬜ |
| 2.8 | Hook `process.on('exit')` in `bootstrapper.js` to actively kill all spawned child processes (language servers, debuggers) — no orphans | Ext Host | High | ⬜ |

**Release 2 exit criteria:** No `_popen`, no `eval`, no direct extension DOM `innerHTML`, no hardcoded pipe name; security scan passes.

---

## Phase 3 — Stability & Correctness (Release 3)

**Goal:** Fix deadlocks, leaks, races, and broken IPC promise resolution.

| # | Task | Area | Priority | Done? |
|---|------|------|----------|-------|
| 3.1 | Fix `TerminalProcess::stop()` / `TerminalService::shutdown()` — use `CancelIoEx` or async detach instead of joining a thread blocked on `ReadFile` | Core Terminal | Critical | ⬜ |
| 3.2 | Fix `TerminalService` / `DebugAdapterService` — remove dead `PtySession`/`DebugSession` pointers from `m_sessions` proactively to stop unbounded memory leaks | Core Terminal/Debug | Critical | ⬜ |
| 3.3 | Fix `DebugAdapterService::shutdown()` — condition variables instead of synchronous `join()` on `sleep_for(500ms)` threads | Core Debug | High | ⬜ |
| 3.4 | Fix IPC promise leak — C++ must call the webview's promise resolver after sending a reply; every `sendIpcMessage` await must resolve | Core IPC + UI | Critical | ⬜ |
| 3.5 | Fix `ExtensionManager` / C++ split-brain — remove C++ shell-script extension management; route all install/uninstall/enable/disable over IPC to Node.js `ExtensionManager` | Core Plugins + Ext Host | High | ⬜ |
| 3.6 | Fix Lit component event listener `this` binding — use arrow functions / explicit bind in `vrutti-explorer-view.ts` and all components | UI Frontend | High | ⬜ |
| 3.7 | Resolve all paths dynamically from executable directory — remove hardcoded `d:/vrutti/...` and `src\\ext\\bin\\node.exe` | App Entry | High | ⬜ |

**Release 3 exit criteria:** No shutdown deadlock; no leaked pending promises; no unbounded session growth; runs from arbitrary install directory.

---

## Phase 4 — Backend Service Completeness (Release 4)

**Goal:** Convert stub plugins into functional backends. Backend-first.

| # | Task | Area | Priority | Done? |
|---|------|------|----------|-------|
| 4.1 | `TerminalService` — implement real ConPTY (Windows) / forkpty (POSIX); reader thread streams raw bytes to UI without mock exit | Core Terminal | Critical | ⬜ |
| 4.2 | `DebugAdapterService` — implement real DAP Content-Length framing, spawn adapter processes (node/python), stdin/stdout RPC streaming | Core Debug | High | ⬜ |
| 4.3 | `LanguageFeatureService` — wire completion/hover requests to ExtHost over RPC instead of returning empty arrays | Core Plugins | High | ⬜ |
| 4.4 | `DiagnosticService` — fully parse `IMarkerData` fields (severity, message, start/end line/col) and emit changed events to UI | Core Plugins | Medium | ⬜ |
| 4.5 | `WorkspaceService` — implement `update_workspace_folders` (add/remove folders), broadcast workspace topology change over RPC | Core Plugins | Medium | ⬜ |
| 4.6 | `FileSystemService` — return actual file content (base64-encoded) instead of `"..."`; add file watcher (`ReadDirectoryChangesW` / `inotify`) | Core Plugins | High | ⬜ |
| 4.7 | `ExtensionManager` — implement install/uninstall/enable/disable lifecycle commands (backed by `ExtensionScanner` + `VSXRegistryService`) | Core Plugins | High | ⬜ |
| 4.8 | `DocumentSyncService` — implement flush of pending edits to ExtHost (`ExtHostDocumentsShape.$acceptModelChanged`) on 50ms debounce | Core Editor | Medium | ⬜ |

**Release 4 exit criteria:** Terminal actually spawns a shell and streams output; DAP can start/stop a session; FS returns real file contents; extension list is functional.

---

## Phase 5 — Extension Host & Protocol Parity (Release 5)

**Goal:** Make the Node.js extension host bridge robust and complete the JSON-RPC router.

| # | Task | Area | Priority | Done? |
|---|------|------|----------|-------|
| 5.1 | Complete `VSCodeProtocol` router — map all `MainThread*` messages (documents, editors, commands, output, terminal, debug, diagnostics) instead of the 2-method stub | Core IPC | High | ⬜ |
| 5.2 | Fix `ipc.js` — resolve all pending promises on disconnect; handle `EPIPE`/`ECONNRESET` without `process.exit(0)` aborting pending requests | Ext Host | High | ⬜ |
| 5.3 | Fix `dap-client.js` `Content-Length` regex — `(\\d+)` → `\\s*:\\s*(\\d+)` to allow flexible spacing | Ext Host | Medium | ⬜ |
| 5.4 | Remove duplicate C++ `DebugAdapterService`; rely purely on Node `dap-client.js` (resolve split-brain) | Core Debug | High | ⬜ |
| 5.5 | Implement `lsp-client.js` → `LanguageFeatureService` routing for IntelliSense (completions, hover, definition) | Ext Host + Core | Medium | ⬜ |
| 5.6 | Implement `textmate-engine.js` grammar tokenization wired to frontend editor syntax highlighting | Ext Host + UI | Medium | ⬜ |
| 5.7 | Implement `theme-resolver.js` mapping VS Code theme files → frontend CSS variables | Ext Host + UI | Medium | ⬜ |

**Release 5 exit criteria:** ExtHost connects, exchanges init data, and responds to document/editor commands; LSP and DAP clients functional.

---

## Phase 6 — Frontend Shell & Editor (Release 6)

**Goal:** Make the Lit frontend a real editor shell wired to the C++ backend.

| # | Task | Area | Priority | Done? |
|---|------|------|----------|-------|
| 6.1 | Wire CodeMirror 6 to C++ `DocumentSyncService` over IPC — real text sync (not mock) | UI Frontend | Critical | ⬜ |
| 6.2 | Implement split editor groups (`editor-group` with drag-to-rearrange) — currently single editor + fixed terminal panel | UI Frontend | High | ⬜ |
| 6.3 | Implement save-as dialog (currently `window.alert('Save As not implemented')`) | UI Frontend | High | ⬜ |
| 6.4 | Implement search & replace UI backed by native `src/plugins/search/` plugin (ripgrep) | UI Frontend | High | ⬜ |
| 6.5 | Implement problems/diagnostics panel consuming `DiagnosticService` events | UI Frontend | High | ⬜ |
| 6.6 | Implement settings UI that reads/writes via `SettingsManager` IPC (not just `localStorage`) | UI Frontend | Medium | ⬜ |
| 6.7 | Implement status bar items (dynamic, from `statusbar/show|update|hide` IPC) | UI Frontend | Medium | ⬜ |
| 6.8 | Implement quick-pick / command palette fully backed by `CommandHandlerService` | UI Frontend | Medium | ⬜ |

**Release 6 exit criteria:** User can open a file, edit text, sync to backend, split editor, save-as, search, and see diagnostics.

---

## Phase 7 — Extension UI & Views (Release 7)

**Goal:** Extensions can contribute views, activity bar items, menus, and commands safely.

| # | Task | Area | Priority | Done? |
|---|------|------|----------|-------|
| 7.1 | Extension sidebar view: list installed extensions, install from Open VSX, uninstall, enable/disable | UI Frontend | High | ⬜ |
| 7.2 | Extension contributions: `activitybar`, `views`, `menus`, `commands`, `keybindings` registered into `Registry.ts` via IPC | UI Frontend | High | ⬜ |
| 7.3 | Extension details page (README, changelog, dependencies) | UI Frontend | Medium | ⬜ |
| 7.4 | SCM / Git view (currently `vrutti-scm.ts`, `vrutti-git-graph.ts`) — wire to real git operations via C++ `fs/` + process spawning | UI Frontend | Medium | ⬜ |
| 7.5 | Output panel and debug console wired to `TerminalService` / `DebugAdapterService` | UI Frontend | Medium | ⬜ |

**Release 7 exit criteria:** User can browse, install, and manage extensions; extension-contributed views appear in sidebar/activity bar.

---

## Phase 8 — Advanced Editor Features (Release 8)

**Goal:** Parity with core VS Code editing experience.

| # | Task | Area | Priority | Done? |
|---|------|------|----------|-------|
| 8.1 | Minimap (`@replit/codemirror-minimap` already in package.json — wire it) | UI Frontend | Medium | ⬜ |
| 8.2 | Breadcrumbs (symbol navigation) | UI Frontend | Medium | ⬜ |
| 8.3 | Diff editor wiring (`vrutti-diff-editor.ts` stub → real diff service) | UI Frontend | Medium | ⬜ |
| 8.4 | Find/replace widget in-editor | UI Frontend | Medium | ⬜ |
| 8.5 | Multi-cursor and selection operations | UI Frontend | Medium | ⬜ |
| 8.6 | Terminal tabs and multiple terminal instances (xterm.js) | UI Frontend | Medium | ⬜ |

**Release 8 exit criteria:** Editor has minimap, breadcrumbs, find/replace, diff editor, multi-terminal tabs.

---

## Phase 9 — Platform Services (Release 9)

**Goal:** Add missing VS Code platform services.

| # | Task | Area | Priority | Done? |
|---|------|------|----------|-------|
| 9.1 | Secret storage / credentials (keytar / Credential Manager) for extension auth | Core | Medium | ⬜ |
| 9.2 | Notifications service (toast) for extensions and system | Core + UI | Medium | ⬜ |
| 9.3 | Progress service for long operations | Core + UI | Medium | ⬜ |
| 9.4 | Quick input (input box, password prompt) | UI Frontend | Medium | ⬜ |
| 9.5 | Crash reporter + telemetry pipeline | Core | Low | ⬜ |
| 9.6 | Auto-update check/download/installer integration | Core + Installer | Low | ⬜ |

**Release 9 exit criteria:** Notifications, progress, quick input, secret storage functional.

---

## Phase 10 — Polish & Release Packaging (Release 10)

**Goal:** Compilable, distributable dev-stage app.

| # | Task | Area | Priority | Done? |
|---|------|------|----------|-------|
| 10.1 | Welcome page | UI Frontend | Low | ⬜ |
| 10.2 | Zen mode / center layout | UI Frontend | Low | ⬜ |
| 10.3 | Final `product.json` values (version, commit, URLs) | Build | Medium | ⬜ |
| 10.4 | `package_release.ps1` → portable ZIP | Build | Medium | ⬜ |
| 10.5 | `build_installer.ps1` → Inno Setup 6 `.exe` installer | Build | Medium | ⬜ |
| 10.6 | End-to-end test: launch `vrutti_app.exe`, open folder, edit file, open terminal, install extension, close cleanly | QA | Critical | ⬜ |

**Release 10 exit criteria:** Portable ZIP and installer build successfully; app launches and completes core workflow without crashes.

---

## Release Summary (10 Releases)

| Release | Focus | Key Deliverable |
|---------|-------|-----------------|
| 1 | Foundation & Build Hygiene | Compiling C++ core + frontend; JSON lib; fixed settings/IPC/plugin races |
| 2 | Security Hardening | No RCE/XSS; per-instance pipes; no orphan processes |
| 3 | Stability & Correctness | No deadlocks; no promise leaks; no session memory leaks |
| 4 | Backend Service Completeness | Real PTY, DAP, LSP, FS content, extension lifecycle |
| 5 | Extension Host & Protocol Parity | Full JSON-RPC router; ExtHost stable; duplicate DAP removed |
| 6 | Frontend Shell & Editor | Real editor sync; split editors; save-as; search; problems |
| 7 | Extension UI & Views | Extension marketplace sidebar; contributions registered |
| 8 | Advanced Editor Features | Minimap, breadcrumbs, diff editor, find/replace, terminal tabs |
| 9 | Platform Services | Secrets, notifications, progress, quick input, telemetry |
| 10 | Polish & Release Packaging | Welcome page, zen mode, ZIP + installer, E2E test pass |

---

## Extension to 10–20 Releases (Optional Scope)

If additional releases are desired beyond the core 10, split large phases further:

- **Release 11:** Remote development (SSH/WSL/tunnel)
- **Release 12:** User data sync / profiles
- **Release 13:** Notebook editor
- **Release 14:** Merge editor
- **Release 15:** Source control timeline
- **Release 16:** Markdown previewer
- **Release 17:** Custom editors / webview extensions
- **Release 18:** Accessibility (screen reader, ARIA, high contrast)
- **Release 19:** Sandboxed extension iframe hardening + CSP
- **Release 20:** Final QA, performance tuning, release candidate

---

## Backend-to-Frontend Dependency Rule

Every release must follow this order within the phase:
1. **Core C++ plugin** (implement the real service, not stub)
2. **IPC protocol** (add JSON-RPC methods to `VSCodeProtocol` + `ipc.js`)
3. **Extension Host** (Node side, if applicable)
4. **Frontend Lit component** (consume IPC, render UI)
5. **Tests** (unit test C++ plugin; e2e test the flow)

No frontend task is started until its backend plugin and IPC method exist.
