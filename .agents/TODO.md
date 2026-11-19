# Vrutti IDE — .agents/ TODO

**Scope:** `vrutti_ide/.agents/` only. Parent `vrutti/` never touched.

## Purpose
Agent/organizational configuration: instructions and structure docs for AI agents working on this repository.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `AGENTS.md` | Agent instructions | Present |
| `structure.md` | Directory structure docs | Present |

## Tasks (Priority Order)
- [ ] **AGT1.** Update `AGENTS.md` with the new directory structure (DI, commands, configuration, storage, theme, telemetry, notifications, updates, secrets, layout, workbench, services, protocols, api, scripts, docs).
- [ ] **AGT2.** Add instruction: never modify parent `vrutti/` directory.
- [ ] **AGT3.** Add instruction: all new features get a `TODO.md` in their directory.
- [ ] **AGT4.** Add instruction: backend-first implementation order (C++ plugin → IPC → frontend).
- [ ] **AGT5.** Add instruction: security rules (no `_popen`, no `eval`, no direct extension DOM injection).

## Related
- `TODO.md` (root) — project overview
- `architecture.md` — architecture reference
