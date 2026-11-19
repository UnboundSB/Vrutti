# Vrutti IDE — VS Code Extension Compatibility Mandate

**Status:** Architectural Decision Record (ADR) — Hard Requirement  
**Scope:** `vrutti_ide/` only. Parent `vrutti/` never touched.

## The Mandate

**If a VS Code-compatible extension works in VS Code, it must work in Vrutti.**  
This is non-negotiable. It means Vrutti must implement the full `vscode` API surface that extensions rely on, not a subset.

This changes the priority order completely. The Extension Host bridge and `vscode` API shim are **Tier 0 foundation**, not Tier 5 nice-to-haves. The editor, terminal, settings, and UI all exist to serve extensions.

---

## What VS Code Extensions Actually Need

Extensions are written against the `vscode` API contract defined in `vscode.d.ts` (parent `vrutti/src/vscode-dts/`, 179 proposed `.d.ts` files). They expect:

| API Surface | Extension Use Case | Vrutti Status |
|-------------|-------------------|---------------|
| `vscode.window` | showInformationMessage, createTerminal, createWebviewPanel, showQuickPick | ❌ Not shimmed |
| `vscode.workspace` | getWorkspaceFolder, openTextDocument, findFiles, saveAll | ⚠️ Partial (WorkspaceService stub) |
| `vscode.commands` | registerCommand, executeCommand | ⚠️ Partial (CommandHandlerService stub) |
| `vscode.extensions` | getExtension, all | ❌ Not shimmed |
| `vscode.languages` | registerCompletionItemProvider, registerHoverProvider | ❌ Not shimmed |
| `vscode.debug` | startDebugging, registerDebugAdapterDescriptorFactory | ❌ Not shimmed |
| `vscode.uri` | file(), joinPath(), parse() | ❌ Not shimmed |
| `vscode.Configuration` | get(), has(), update() | ⚠️ Partial (SettingsManager stub) |
| `vscode.Memento` | get(), set(), keys() | ❌ Not implemented |
| `vscode.SecretStorage` | get(), store(), delete() | ❌ Not implemented |
| `vscode.WebviewPanel` | Custom extension UIs (gitlens, etc.) | ❌ Not implemented |
| `vscode.Terminal` | sendText(), show(), hide(), create() | ⚠️ Partial (TerminalService stub) |
| `vscode.TextDocument` | uri, lineCount, positionAt, getText() | ⚠️ Partial (DocumentSyncService) |
| `vscode.Decoration` | Text editor decorations | ❌ Not implemented |
| `vscode.StatusBarItem` | show(), hide(), text, command | ⚠️ Partial (Registry.ts) |
| `vscode.TreeView` | Custom tree views (explorer, scm, debug) | ❌ Not implemented |
| `vscode.ThemeColor` / `vscode.ThemeIcon` | Theming | ❌ Not implemented |

Extensions also depend on `package.json` `contributes`:
- `commands`, `keybindings`, `menus` — ⚠️ Partial
- `views`, `viewsContainers` — ⚠️ Partial
- `languages`, `grammars` — ❌ Not implemented
- `themes`, `iconThemes` — ❌ Not implemented
- `debuggers` — ❌ Not implemented
- `problemMatchers` — ❌ Not implemented
- `typescript`, `jsonValidation` — ❌ Not implemented
- `customEditors` — ❌ Not implemented
- `notebookRenderer` — ❌ Not implemented
- `walkthroughs`, `welcomePage` — ❌ Not implemented

---

## What Vrutti Already Has (The Foundation)

These are real and can be leveraged:

| Asset | How it helps extension compatibility |
|-------|-------------------------------------|
| `src/ext/bootstrapper.js` | Spawns upstream VS Code `extHostProcess.js` — extensions run in the real Extension Host |
| `src/ext/ipc.js` | JSON-RPC client over named pipe to C++ core |
| `src/core/ipc/VSCodeProtocol.cpp` | Receives `MainThreadShape` messages from ExtHost |
| `src/core/plugins/VSCodeExtHostService.cpp` | Builds `IExtensionHostInitData` (partial) |
| `src/core/plugins/ExtensionScanner.cpp` | Reads extension `package.json` manifests |
| `src/core/plugins/VSXRegistryService.cpp` | Open VSX search |
| `src/core/editor/PieceTable.cpp` | Text document model |
| `src/ui/frontend/src/core/Registry.ts` | Contribution registry for UI parts |
| `vscode-dts/` (parent) | The API contract definition (read-only reference) |

---

## What Must Be Built (Extension Compatibility Layer)

### 1. Complete `vscode` API Shim Layer (`src/ext/api/`)

Extensions call `vscode.window.showInformationMessage()`. This shim must intercept that call, convert it to an IPC message, send it to the C++ core, and return a Promise.

| Shim File | `vscode` API to Cover | IPC Method |
|-----------|----------------------|------------|
| `window-api.js` | `showInformationMessage`, `showWarningMessage`, `showErrorMessage`, `showQuickPick`, `showInputBox`, `createTerminal`, `createWebviewPanel`, `setStatusBarMessage` | `notification/show`, `terminal/create`, `webview/create` |
| `workspace-api.js` | `getWorkspaceFolder`, `workspaceFolders`, `openTextDocument`, `saveAll`, `findFiles`, `getConfiguration` | `workspace/folders`, `document/open`, `configuration/get` |
| `commands-api.js` | `registerCommand`, `executeCommand`, `getCommands` | `command/register`, `command/execute`, `command/get` |
| `extensions-api.js` | `getExtension`, `all`, `extensionPath` | `extensions/list`, `extensions/get` |
| `languages-api.js` | `registerCompletionItemProvider`, `registerHoverProvider`, `registerDefinitionProvider` | `language/completion`, `language/hover` |
| `debug-api.js` | `startDebugging`, `registerDebugAdapterDescriptorFactory` | `debug/start`, `dap/start` |
| `uri-api.js` | `file()`, `joinPath()`, `parse()` | Local URI manipulation (no IPC needed) |
| `configuration-api.js` | `get()`, `has()`, `update()` | `configuration/get`, `configuration/set` |
| `memento-api.js` | `get()`, `set()`, `keys()` | `memento/get`, `memento/set` |
| `secret-storage-api.js` | `get()`, `store()`, `delete()` | `secrets/get`, `secrets/store` |
| `webview-api.js` | `createWebviewPanel`, `webview.html`, `webview.onDidMessage` | `webview/create`, `webview/message` |
| `terminal-api.js` | `sendText()`, `show()`, `hide()`, `create()` | `terminal/write`, `terminal/show` |
| `textdocument-api.js` | `uri`, `lineCount`, `positionAt`, `getText()` | `document/get` |
| `decoration-api.js` | Text editor decorations | `editor/decoration/set` |
| `treeview-api.js` | `createTreeView` | `treeview/register` |

### 2. Complete JSON-RPC Protocol (`src/ext/protocols/`)

`methods.json` must define every IPC method with:
- Direction (C++→Node, Node→C++, bi-di)
- Params schema (JSON Schema)
- Result schema
- Example payload

This is the single source of truth. C++ `VSCodeProtocol` and Node `ipc.js` must both implement from this.

### 3. Complete `IExtensionHostInitData` (`VSCodeExtHostService.cpp`)

The current init data is partial. It must include:
- `workspace` — full workspace folder list with URIs
- `environment` — appRoot, globalStorageHome, workspaceStorageHome, logsLocation
- `extensions` — all installed extensions with their `package.json` `contributes`
- `telemetryInfo` — sessionId, machineId
- `languageStatus` — registered languages
- `configuration` — initial settings snapshot

### 4. WebView Panel Support (`src/core/webview/` — NEW DIRECTORY)

Extensions like GitLens create custom webviews. Vrutti needs:
- C++ `WebViewPanelService` — creates native WebView2 windows for extension webviews
- IPC methods: `webview/create`, `webview/setHtml`, `webview/postMessage`, `webview/onDidMessage`
- Frontend `vrutti-webview` component already exists but needs wiring

### 5. Extension `contributes` Processing

When an extension is installed, its `package.json` `contributes` must be translated into Vrutti concepts:

| `contributes` | Vrutti Mapping |
|--------------|----------------|
| `commands` | Register in `CommandHandlerService` + frontend `Registry.ts` |
| `keybindings` | Register in frontend keybinding registry |
| `menus` | Register in frontend menu registry |
| `views` / `viewsContainers` | Register views in sidebar/panel/activitybar |
| `languages` | Register language ID for syntax highlighting |
| `grammars` | Register TextMate grammar with `textmate-engine.js` |
| `themes` | Register theme with `core/theme/` |
| `debuggers` | Register DAP adapter with `dap-client.js` |
| `problemMatchers` | Register problem matcher for terminal output |

---

## Reprioritized Build Order (Extension Compatibility First)

### Phase 0 — Foundation (ALREADY CODED, NOT COMPILED)
Same as before — `nlohmann/json`, `SettingsManager`, `PluginLoader` mutex, per-instance pipe, dynamic `node.exe` path. **Must compile before anything else.**

### Phase 1 — Extension API Shim (NEW TOP PRIORITY)

| # | Task | File/Dir |
|---|------|----------|
| 1.1 | Build `src/ext/protocols/methods.json` — complete IPC method catalog | `src/ext/protocols/` |
| 1.2 | Build `src/ext/api/vscode-shim.js` — main `vscode` object with all API entry points | `src/ext/api/` |
| 1.3 | Build `src/ext/api/window-api.js` — `vscode.window` shim | `src/ext/api/` |
| 1.4 | Build `src/ext/api/workspace-api.js` — `vscode.workspace` shim | `src/ext/api/` |
| 1.5 | Build `src/ext/api/commands-api.js` — `vscode.commands` shim | `src/ext/api/` |
| 1.6 | Build `src/ext/api/extensions-api.js` — `vscode.extensions` shim | `src/ext/api/` |
| 1.7 | Build `src/ext/api/languages-api.js` — `vscode.languages` shim | `src/ext/api/` |
| 1.8 | Build `src/ext/api/debug-api.js` — `vscode.debug` shim | `src/ext/api/` |
| 1.9 | Build `src/ext/api/configuration-api.js` — `vscode.Configuration` shim | `src/ext/api/` |
| 1.10 | Build `src/ext/api/memento-api.js` — `vscode.Memento` shim | `src/ext/api/` |
| 1.11 | Build `src/ext/api/secret-storage-api.js` — `vscode.SecretStorage` shim | `src/ext/api/` |
| 1.12 | Build `src/ext/api/webview-api.js` — `vscode.WebviewPanel` shim | `src/ext/api/` |
| 1.13 | Build `src/ext/api/terminal-api.js` — `vscode.Terminal` shim | `src/ext/api/` |
| 1.14 | Build `src/ext/api/textdocument-api.js` — `vscode.TextDocument` shim | `src/ext/api/` |
| 1.15 | Build `src/ext/api/decoration-api.js` — `vscode.Decoration` shim | `src/ext/api/` |
| 1.16 | Build `src/ext/api/treeview-api.js` — `vscode.TreeView` shim | `src/ext/api/` |
| 1.17 | Build `src/ext/api/unimplemented.js` — graceful fallback for any API not yet implemented | `src/ext/api/` |

**Exit criterion:** A test extension can call `vscode.window.showInformationMessage('hello')` and see a toast in Vrutti.

### Phase 2 — Protocol & Init Data Parity

| # | Task | File/Dir |
|---|------|----------|
| 2.1 | Complete `VSCodeProtocol.cpp` — full `MainThreadShape` router | `src/core/ipc/` |
| 2.2 | Complete `IExtensionHostInitData` in `VSCodeExtHostService.cpp` | `src/core/plugins/` |
| 2.3 | Wire `ipc.js` pending-request resolution on disconnect | `src/ext/` |
| 2.4 | Fix DAP `Content-Length` regex (`\\s*:\\s*(\\d+)`) | `src/ext/dap-client.js` |
| 2.5 | Remove duplicate C++ `DebugAdapterService` | `src/core/plugins/` |

### Phase 3 — WebView Panel (Extension Custom UIs)

| # | Task | File/Dir |
|---|------|----------|
| 3.1 | Create `src/core/webview/` directory + `WebViewPanelService.cpp` | `src/core/webview/` |
| 3.2 | Implement WebView2 panel creation via IPC | `src/core/webview/` |
| 3.3 | Wire frontend `vrutti-webview.ts` to IPC `webview/*` methods | `src/ui/frontend/src/components/` |

### Phase 4 — Extension `contributes` Processing

| # | Task | File/Dir |
|---|------|----------|
| 4.1 | Process `contributes.commands` → register in CommandHandlerService + Registry.ts | `src/core/plugins/ExtensionManager.cpp` |
| 4.2 | Process `contributes.keybindings` → frontend keybinding registry | `src/ui/frontend/src/core/Registry.ts` |
| 4.3 | Process `contributes.menus` → frontend menu registry | `src/ui/frontend/src/core/Registry.ts` |
| 4.4 | Process `contributes.views` / `viewsContainers` → sidebar/panel/activitybar | `src/ui/frontend/src/components/layout/` |
| 4.5 | Process `contributes.languages` + `grammars` → TextMate engine | `src/ext/textmate-engine.js` |
| 4.6 | Process `contributes.themes` → core theme service | `src/core/theme/` |
| 4.7 | Process `contributes.debuggers` → DAP adapter registration | `src/ext/dap-client.js` |

### Phase 5 — Backend Services (for extension support)

| # | Task | File/Dir |
|---|------|----------|
| 5.1 | Real PTY terminal | `src/core/terminal/` |
| 5.2 | Real filesystem content | `src/core/plugins/FileSystemService.cpp` |
| 5.3 | Real workspace folders | `src/core/plugins/WorkspaceService.cpp` |
| 5.4 | Real language features (LSP wiring) | `src/core/plugins/LanguageFeatureService.cpp` |
| 5.5 | Real diagnostics | `src/core/plugins/DiagnosticService.cpp` |
| 5.6 | Extension lifecycle (install/uninstall/activate) | `src/core/plugins/ExtensionManager.cpp` |

### Phase 6 — Frontend Shell

| # | Task | File/Dir |
|---|------|----------|
| 6.1 | Layout parts (activitybar, sidebar, panel, editor groups, statusbar, titlebar) | `src/ui/frontend/src/components/layout/` |
| 6.2 | Workbench shell composition | `src/ui/frontend/src/components/workbench/` |
| 6.3 | Real editor sync (CodeMirror ↔ Piece Table) | `src/ui/frontend/src/components/vrutti-editor.ts` |
| 6.4 | Split editor groups | `vrutti-editor-layout.ts` |
| 6.5 | Save-as dialog | Frontend |
| 6.6 | Search & replace UI | `vrutti-search.ts` |
| 6.7 | Problems panel | `vrutti-problems-view.ts` |

### Phase 7 — Advanced Features & Packaging

Minimap, breadcrumbs, diff editor, telemetry, crash reporter, auto-update, secrets, notifications, welcome page, Inno Setup installer.

---

## Extension Compatibility Test Suite

Every release must pass these tests:

| Test | Description |
|------|-------------|
| **EC1** | `vscode.window.showInformationMessage()` → toast appears |
| **EC2** | `vscode.workspace.workspaceFolders` → returns correct folders |
| **EC3** | `vscode.commands.registerCommand()` + `executeCommand()` → command runs |
| **EC4** | `vscode.extensions.getExtension('publisher.name')` → returns extension object |
| **EC5** | `vscode.languages.registerCompletionItemProvider()` → completions appear |
| **EC6** | `vscode.debug.startDebugging()` → DAP session starts |
| **EC7** | `vscode.window.createWebviewPanel()` → webview panel opens |
| **EC8** | `vscode.Terminal` → `sendText()` writes to PTY |
| **EC9** | Extension `package.json` `contributes.views` → appears in sidebar |
| **EC10** | Extension `package.json` `contributes.themes` → theme selectable |

A test extension (in `src/ext/test_exts/`) should exercise all 10 tests on every release.

---

## Hard Constraints

1. **Parent `vrutti/` is never touched.** All extension compatibility code lives in `vrutti_ide/`.
2. **Backend → IPC → Frontend.** No API shim method is exposed until its C++ IPC handler exists.
3. **Graceful degradation.** Unimplemented `vscode` APIs must log a warning and return a sensible default (undefined, empty array, etc.) — never crash.
4. **Single protocol source of truth.** `src/ext/protocols/methods.json` governs both C++ and Node implementations.

---

## Immediate Next Steps

1. Write `src/ext/protocols/methods.json` (the IPC method catalog).
2. Write `src/ext/api/vscode-shim.js` (main entry point).
3. Write `src/ext/api/window-api.js` (first real API shim).
4. Complete `VSCodeProtocol.cpp` router (C++ side).
5. Write `EC1` test: `showInformationMessage` end-to-end.

All work stays inside `vrutti_ide/`.