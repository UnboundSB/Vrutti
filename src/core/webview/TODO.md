# Vrutti IDE — src/core/webview/ TODO

**Scope:** `vrutti_ide/src/core/webview/` only. Parent `vrutti/` never touched.

## Purpose
WebView panel service for extension custom UIs. Extensions like GitLens create webviews via `vscode.window.createWebviewPanel()`. Vrutti needs native WebView2 panels backed by IPC.

## WHY THIS IS NEW DIRECTORY
Extensions need custom webview panels for rich UI (git graphs, notebooks, settings pages, etc.). Without this, many VS Code extensions cannot function.

## Planned Files
| File | Purpose |
|------|---------|
| `WebViewPanelService.cpp/.h` | Creates/manages native WebView2 panels for extensions |
| `WebViewPanel.cpp/.h` | Individual webview panel lifecycle (html, messages, dispose) |
| `structure.md` | Directory docs |

## Tasks (Priority Order — Extension Compatibility)
- [ ] **WB1.** Create `WebViewPanelService` as C++ plugin DLL (`createPlugin`/`destroyPlugin`)
- [ ] **WB2.** Implement `webview/create` IPC — spawn native WebView2 panel with unique panelId
- [ ] **WB3.** Implement `webview/setHtml` IPC — set panel HTML content
- [ ] **WB4.** Implement `webview/postMessage` IPC — C++ → extension webview messages
- [ ] **WB5.** Implement `webview/onDidMessage` IPC — extension webview → C++ messages
- [ ] **WB6.** Implement panel dispose/cleanup on extension deactivation
- [ ] **WB7.** Wire frontend `vrutti-webview.ts` component to IPC `webview/*` methods
- [ ] **WB8.** Add panel persistence (position, size) across restarts

## Related
- `src/ext/api/webview-api.js` — Node-side `vscode.WebviewPanel` shim
- `src/ui/frontend/src/components/vrutti-webview.ts` — existing webview component stub
- `src/core/ipc/TODO.md` — IPC `webview/*` methods
- `docs/EXTENSION_COMPATIBILITY.md` — compatibility mandate
