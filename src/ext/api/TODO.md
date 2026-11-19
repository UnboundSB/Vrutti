# Vrutti IDE — src/ext/api/ TODO

**Scope:** `vrutti_ide/src/ext/api/` only. Parent `vrutti/` never touched.

## Purpose
VS Code API shim layer for extensions. Provides the `vscode` API surface that extensions call, shimming unsupported APIs gracefully.

## HARD REQUIREMENT: Extension Compatibility
**If it works in VS Code, it must work in Vrutti.** This shim layer is Tier 0 foundation, not Tier 5 nice-to-have.

## API Shim Files (Map to vscode.d.ts)
| File | vscode API | Priority | Status |
|------|-----------|----------|--------|
| `vscode-shim.js` | Main `vscode` object, exports all namespaces | Tier 1 | Empty |
| `window-api.js` | `vscode.window.*` (showInformationMessage, createTerminal, createWebviewPanel, showQuickPick, showInputBox, setStatusBarMessage) | Tier 1 | Empty |
| `workspace-api.js` | `vscode.workspace.*` (getWorkspaceFolder, workspaceFolders, openTextDocument, saveAll, findFiles, getConfiguration) | Tier 1 | Empty |
| `commands-api.js` | `vscode.commands.*` (registerCommand, executeCommand, getCommands) | Tier 1 | Empty |
| `extensions-api.js` | `vscode.extensions.*` (getExtension, all, extensionPath) | Tier 1 | Empty |
| `languages-api.js` | `vscode.languages.*` (registerCompletionItemProvider, registerHoverProvider, registerDefinitionProvider) | Tier 1 | Empty |
| `debug-api.js` | `vscode.debug.*` (startDebugging, registerDebugAdapterDescriptorFactory) | Tier 1 | Empty |
| `configuration-api.js` | `vscode.Configuration` (get, has, update) | Tier 1 | Empty |
| `memento-api.js` | `vscode.Memento` (get, set, keys) | Tier 1 | Empty |
| `secret-storage-api.js` | `vscode.SecretStorage` (get, store, delete) | Tier 1 | Empty |
| `webview-api.js` | `vscode.WebviewPanel` (createWebviewPanel, webview.html, onDidMessage) | Tier 3 | Empty |
| `uri-api.js` | `vscode.Uri` (file, joinPath, parse) | Tier 1 | Empty |
| `terminal-api.js` | `vscode.Terminal` (sendText, show, hide, create) | Tier 1 | Empty |
| `textdocument-api.js` | `vscode.TextDocument` (uri, lineCount, positionAt, getText) | Tier 1 | Empty |
| `decoration-api.js` | `vscode.Decoration` | Tier 2 | Empty |
| `treeview-api.js` | `vscode.TreeView` (createTreeView) | Tier 2 | Empty |
| `unimplemented.js` | Graceful fallback: log warning, return sensible default | Tier 1 | Empty |

## Tasks (Priority Order — Extension Compatibility First)
- [ ] **API1.** Create `vscode-shim.js` — main entry point that aggregates all namespace shims and exposes `vscode` global
- [ ] **API2.** Create `window-api.js` — `showInformationMessage` → IPC `notification/show`, returns Promise
- [ ] **API3.** Create `workspace-api.js` — `workspaceFolders` → IPC `workspace/folders`, returns array
- [ ] **API4.** Create `commands-api.js` — `registerCommand`/`executeCommand` → IPC `command/register` / `command/execute`
- [ ] **API5.** Create `extensions-api.js` — `getExtension` → IPC `extensions/get`, `all` → `extensions/list`
- [ ] **API6.** Create `languages-api.js` — `registerCompletionItemProvider` → IPC `language/completion/register`
- [ ] **API7.** Create `debug-api.js` — `startDebugging` → IPC `debug/start`
- [ ] **API8.** Create `configuration-api.js` — `get`/`set` → IPC `configuration/get` / `configuration/set`
- [ ] **API9.** Create `memento-api.js` — `get`/`set`/`keys` → IPC `memento/*`
- [ ] **API10.** Create `secret-storage-api.js` — `get`/`store`/`delete` → IPC `secrets/*`
- [ ] **API11.** Create `uri-api.js` — `file()`/`joinPath()`/`parse()` — local only, no IPC
- [ ] **API12.** Create `terminal-api.js` — `sendText()`/`show()`/`hide()`/`create()` → IPC `terminal/*`
- [ ] **API13.** Create `textdocument-api.js` — `uri`/`lineCount`/`positionAt`/`getText()` → IPC `document/get`
- [ ] **API14.** Create `decoration-api.js` — editor decorations → IPC `editor/decoration/set`
- [ ] **API15.** Create `treeview-api.js` — `createTreeView` → IPC `treeview/register`
- [ ] **API16.** Create `webview-api.js` — `createWebviewPanel` → IPC `webview/create` (Tier 3, after webview service)
- [ ] **API17.** Create `unimplemented.js` — every unimplemented API logs warning and returns default (never crash)
- [ ] **API18.** Wire all shims into `bootstrapper.js` so extensions receive the `vscode` global on startup
- [ ] **API19.** Write EC1–EC10 extension compatibility tests (see `docs/EXTENSION_COMPATIBILITY.md`)

## Related
- `docs/EXTENSION_COMPATIBILITY.md` — full mandate and gap analysis
- `src/ext/protocols/TODO.md` — IPC method catalog (single source of truth)
- `src/core/ipc/TODO.md` — C++ IPC protocol handlers
- `src/core/plugins/VSCodeExtHostService.cpp` — builds `IExtensionHostInitData`
- `src/core/plugins/CommandHandlerService.cpp` — backend command execution
- `src/core/plugins/WorkspaceService.cpp` — workspace folder queries
