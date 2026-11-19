# Vrutti IDE — src/core/configuration/ TODO

**Scope:** `vrutti_ide/src/core/configuration/` only. Parent `vrutti/` never touched.

## Purpose
Typed configuration service: schema definitions, defaults, change notifications, validation. Broader than `SettingsManager` (which is the persistence layer).

## Why This Exists
VS Code has a `configuration` platform service with strongly-typed settings. Vrutti stores raw JSON in `SettingsManager` without schema validation.

## Planned Files
| File | Purpose |
|------|---------|
| `ConfigurationSchema.h/.cpp` | Schema registry (type, default, description) |
| `ConfigurationService.h/.cpp` | Get/set settings with schema validation |
| `ConfigurationChangeEvent.h` | Change notification event |

## Tasks (Priority Order)
- [ ] **CFG1.** Define configuration schema for editor settings (fontFamily, fontSize, wordWrap, tabSize, etc.).
- [ ] **CFG2.** Implement `ConfigurationService` with schema-validated get/set.
- [ ] **CFG3.** Emit change events when a setting is modified.
- [ ] **CFG4.** Wire `ConfigurationService` to `SettingsManager` for persistence.
- [ ] **CFG5.** Expose configuration via IPC: `configuration/get`, `configuration/set`.
- [ ] **CFG6.** Add per-workspace and per-language configuration overrides.

## Related
- `src/core/config/TODO.md` — SettingsManager persistence
- `src/core/di/TODO.md` — ConfigurationService DI registration
- `src/ui/frontend/src/components/vrutti-settings.ts` — frontend settings UI
