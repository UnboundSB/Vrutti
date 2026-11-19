# Vrutti IDE — Component Model Decision

**Status:** Architectural Decision Record (ADR)  
**Scope:** `vrutti_ide/` only. Parent `vrutti/` never touched.

## The Question

Should we build a React-like dynamic component framework with C++ compatibility (lightweight, component injections, utils) OR just add the injection mechanism to our existing stack?

## Recommendation: Do NOT Build a React-like C++ Framework

**Add the injection mechanism to the existing stack.** Do not build a new UI framework in C++.

### Why Not a React-like C++ Framework

| Concern | Reality |
|---------|---------|
| **C++ has no DOM** | React's virtual DOM, reconciliation, hooks, and suspense are DOM-centric concepts. Building them in C++ means rebuilding a browser engine's rendering layer — months of work for marginal gain. |
| **"Light as fuck" contradiction** | A full UI framework (virtual DOM, scheduler, renderer, JS bridge) is the opposite of lightweight. Lit is already ~6KB and production-grade. |
| **Maintenance burden** | You would own the framework's bugs, performance, memory leaks, and onboarding. VS Code doesn't do this; it uses a DI container + workbench parts + extension host. |
| **C++↔JS bridge complexity** | Making React-like components "C++ compatible" requires object marshaling, lifetime management across the C++/Node boundary, and serialization of component trees. This is extremely complex and error-prone. |
| **Wrong abstraction level** | C++ should own state/documents/PTY/FS. The frontend should own rendering. Mixing them in a single component model blurs boundaries and creates tight coupling. |

### What We Already Have (and Should Use)

Vrutti already has a proven, VS Code-aligned architecture:

```
[ Lit UI (frontend) ]  <-- JSON-RPC IPC -->  [ C++ Backend (IPlugin DLLs) ]  <-- JSON-RPC IPC -->  [ Node.js Extension Host ]
```

| Layer | Technology | Role |
|-------|-----------|------|
| UI rendering | Lit + Vite (already a dependency) | All presentation, DOM, styling |
| UI composition | `Registry.ts` contribution registry | Activity bar, views, panels, menus, commands, keybindings |
| Backend services | C++ `IPlugin` DLLs (already implemented) | Documents, FS, terminal, settings, diagnostics |
| IPC contract | JSON-RPC 2.0 over named pipes | The boundary between layers |
| Extension host | Node.js `extHostProcess` (unmodified upstream) | Runs extensions, LSP, DAP |
| Native overlays | ImGui (already vendored) | Debug overlays, native UI if ever needed |

### Component Injection Mechanism (Added to Existing Stack)

Component injection means: an extension or plugin can contribute UI that appears in the IDE without the core knowing about it in advance.

**Frontend side (Lit):**
- `Registry.ts` already supports: `ActivityBarContribution`, `ViewContribution`, `PanelContribution`, `StatusBarContribution`, `MenuContribution`, `CommandContribution`, `EditorProvider`, `QuickPickProvider`.
- Extensions register contributions via IPC (`extensions/installed` message → `registry.registerView()`, `registerActivityBar()`, etc.).
- Views render via a component tag string (e.g., `vrutti-webview`, `vrutti-tree-view`).

**C++ side (backend):**
- `IPlugin::executeCommand(command, payload)` is the generic dispatch mechanism.
- Plugins register commands via `CommandHandlerService`.
- Commands are triggered from the frontend via IPC `command/execute`.

**The missing piece (to add, not to rebuild):**
1. **DI Container** (`src/core/di/`) — lightweight, VS Code `instantiation`-style, so services declare dependencies instead of being hand-wired.
2. **Layout Parts** (`src/ui/frontend/src/components/layout/`) — activitybar, sidebar, panel, editor groups, statusbar (modeled on VS Code `workbench/browser/parts/`).
3. **Workbench Shell** (`src/ui/frontend/src/components/workbench/`) — composes the parts into the full IDE shell.
4. **Frontend Services** (`src/ui/frontend/src/services/`) — typed IPC clients replacing raw `window.sendIpcMessage` calls.

### Summary

| Approach | Verdict | Rationale |
|----------|---------|-----------|
| React-like C++ component framework | ❌ Reject | Over-engineering, C++ has no DOM, maintenance burden, violates separation of concerns |
| Lit frontend + C++ plugins + JSON-RPC IPC | ✅ Adopt | Already in place, lightweight, VS Code-aligned, maintainable |
| Add DI container + layout parts + workbench shell | ✅ Add | Fills the gaps without building a new framework |

### Immediate Next Steps

1. **Do not write any React-like C++ framework code.**
2. Implement `src/core/di/` — lightweight DI container.
3. Implement `src/ui/frontend/src/components/layout/` — layout parts.
4. Implement `src/ui/frontend/src/components/workbench/` — workbench shell.
5. Implement `src/ui/frontend/src/services/` — typed IPC clients.
6. All component injection goes through `Registry.ts` + `IPlugin::executeCommand`.

## Related
- `architecture.md` — full architecture comparison (Mermaid diagram)
- `src/core/di/TODO.md` — DI container tasks
- `src/ui/frontend/src/components/layout/TODO.md` — layout parts
- `src/ui/frontend/src/components/workbench/TODO.md` — workbench shell
- `src/ui/frontend/src/services/TODO.md` — frontend services
