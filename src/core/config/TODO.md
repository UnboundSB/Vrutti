# Vrutti IDE — src/core/config/ TODO

**Scope:** `vrutti_ide/src/core/config/` only. Parent `vrutti/` never touched.

## Purpose
User settings persistence. Singleton `SettingsManager` with background writer thread (non-blocking disk I/O).

## Files
| File | Purpose | Status |
|------|---------|--------|
| `SettingsManager.cpp/.h` | Settings load/update/save | Fixed (Release 1) |

## Recent Fixes (Release 1 — already applied)
- [x] Removed blocking `m_saveFuture.wait()` while holding mutex
- [x] Added dedicated background writer thread + save queue
- [x] `saveAsync()` is now non-blocking

## Remaining Tasks (Priority Order)
- [ ] **CFG1.** Add typed configuration schema (string, number, boolean, enum) with defaults.
- [ ] **CFG2.** Add change notifications: when a setting changes, emit IPC notification to frontend.
- [ ] **CFG3.** Add config migration: handle old settings.json schema gracefully.
- [ ] **CFG4.** Add per-workspace settings (workspace-specific overrides).
- [ ] **CFG5.** Validate setting values against schema before writing.
- [ ] **CFG6.** Expose settings via IPC: `settings/get`, `settings/set` methods.

## Related
- `src/core/configuration/TODO.md` — NEW: typed config schema service (broader than SettingsManager)
- `src/ui/frontend/src/components/vrutti-settings.ts` — frontend settings UI
- `src/core/ipc/TODO.md` — IPC settings methods
