# Vrutti IDE — src/ui/frontend/ TODO

**Scope:** `vrutti_ide/src/ui/frontend/` only. Parent `vrutti/` never touched.

## Purpose
Lit + Vite TypeScript frontend. Presentation layer only — zero application state. Communicates with C++ core via WebView2 IPC.

## Directory Inventory
| Dir | Purpose | Status |
|-----|---------|--------|
| `src/` | Source TypeScript/Lit components | Mixed |
| `public/` | Static assets | Present |
| `dist/` | Build output | Present |
| `dist_new/` | Alternate build output | Present |
| `package.json` | Dependencies (Lit, CodeMirror, xterm, Vite) | Present |
| `vite.config.ts` | Vite build config | Present |
| `tsconfig.json` | TypeScript config | Present |

## Frontend-Level Tasks (Priority Order)
- [ ] **FE1.** Wire CodeMirror 6 to C++ `DocumentSyncService` via IPC (real text sync).
- [ ] **FE2.** Replace `localStorage` settings with IPC `settings/get`/`settings/set`.
- [ ] **FE3.** Fix base64 UTF-8 decoding: `TextDecoder` instead of `decodeURIComponent(escape(atob()))`.
- [ ] **FE4.** Sanitize all Lit `unsafeHTML` with `DOMPurify` (XSS fix).
- [ ] **FE5.** Sandbox extension CSS/JS injections: iframe + postMessage (no direct DOM `innerHTML`).
- [ ] **FE6.** Fix event listener `this` binding: use arrow functions (memory leak fix).
- [ ] **FE7.** Implement split editor groups.
- [ ] **FE8.** Implement save-as dialog.
- [ ] **FE9.** Implement search & replace UI backed by native search plugin.
- [ ] **FE10.** Implement problems panel from `DiagnosticService` IPC events.

## Related
- `src/ui/frontend/src/TODO.md` — source components
- `src/core/ipc/TODO.md` — IPC protocol
- `src/core/plugins/DocumentSyncService.cpp` — text sync backend
