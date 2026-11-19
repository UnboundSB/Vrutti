# Vrutti IDE — src/core/di/ TODO

**Scope:** `vrutti_ide/src/core/di/` only. Parent `vrutti/` never touched.

## Purpose
Dependency Injection container for the C++ core. Modeled on VS Code's `instantiation` platform service: `createDecorator`, `ServiceCollection`, lazy service instantiation.

## Why This Exists
VS Code's workbench shell is built on a formal DI container. Vrutti currently hand-wires service access via `PluginLoader::getPlugin()` and singletons. This directory formalizes that into a DI container so services declare dependencies explicitly.

## Planned Files
| File | Purpose |
|------|---------|
| `ServiceCollection.h/.cpp` | Register / resolve services |
| `ServiceDecorator.h` | `createDecorator` for service identifiers |
| `DIContainer.h/.cpp` | Main container with lifetime management (singleton / transient) |

## Tasks (Priority Order)
- [ ] **DI1.** Implement `createDecorator<T>(name)` returning a service identifier.
- [ ] **DI2.** Implement `ServiceCollection` with `registerSingleton` / `registerTransient`.
- [ ] **DI3.** Implement `DIContainer::getInstance()` and `container.get<T>(decorator)`.
- [ ] **DI4.** Refactor `PluginLoader` to register plugins into the DI container.
- [ ] **DI5.** Refactor `SettingsManager` to be DI-registered (replace singleton pattern where possible).
- [ ] **DI6.** Add constructor injection support for service classes.

## Related
- `src/core/plugins/PluginLoader.cpp` — will use DI for plugin registration
- `src/core/config/TODO.md` — SettingsManager DI migration
- `architecture.md` — VS Code `instantiation` comparison
